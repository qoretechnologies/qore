# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file UnbounceLeadDeletionRequestsDataProvider.qc Unbounce lead deletion requests data provider

/*  UnbounceLeadDeletionRequestsDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the UnbounceDataProvider module
public namespace UnbounceDataProvider {

#! Unbounce lead deletion requests data provider
public class UnbounceLeadDeletionRequestsDataProvider inherits UnbounceDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "lead-deletion-requests",
            "desc": "Unbounce lead deletion requests data provider for retrieving deletion "
                "request status",
            "type": "UnbounceLeadDeletionRequestsDataProvider",
            "supports_read": True,
            "supports_native_search": True,
            "supports_children": True,
            "children_can_support_apis": True,
            "has_record": True,
            "constructor_options": ConstructorOptions,
            "search_options": SearchOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Search options
        const SearchOptions = {
            "page_id": <DataProviderOptionInfo>{
                "display_name": "Page ID",
                "short_desc": "Page to query deletion requests for",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "The page ID to query lead deletion requests for. Required",
                "ref_data": "pages",
            },
            "deletion_request_id": <DataProviderOptionInfo>{
                "display_name": "Deletion Request ID",
                "short_desc": "Get a specific deletion request by ID",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter by deletion request ID for single record lookup",
            },
        };

        #! Child provider map
        const ChildMap = {
            "create": Class::forName("UnbounceCreateLeadDeletionRequestDataProvider"),
        };
    }

    #! Creates the object with a REST client
    constructor(*RestClientIo::RestClientIo rest) : UnbounceDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "lead-deletion-requests";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Unbounce lead deletion requests data provider for retrieving deletion request "
            "status";
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
        return new UnbounceLeadDeletionRequestDataType();
    }

    #! Returns an iterator for records matching the search criteria
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        if (!search_options.page_id) {
            throw "UNBOUNCE-ERROR", "page_id is required for querying lead deletion requests";
        }
        if (!search_options.deletion_request_id) {
            throw "UNBOUNCE-ERROR", "deletion_request_id is required for querying lead "
                "deletion requests";
        }

        hash<auto> result = doGet("pages/" + search_options.page_id
            + "/lead_deletion_request/" + search_options.deletion_request_id);
        return new DefaultRecordIterator(new ListHashIterator((result,)),
            where_cond, search_options);
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace UnbounceDataProvider
