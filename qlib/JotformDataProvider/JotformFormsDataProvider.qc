# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore JotformDataProvider module definition

/*  JotformFormsDataProvider.qc Copyright 2025 - 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the JotformDataProvider module
public namespace JotformDataProvider {

#! JotForm forms collection data provider
public class JotformFormsDataProvider inherits JotformDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "forms",
            "desc": "JotForm forms data provider",
            "type": "JotformFormsDataProvider",
            "supports_read": True,
            "supports_children": True,
            "supports_native_search": True,
            "has_record": True,
            "children_can_support_records": True,
            "search_options": SearchOptions,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Search options
        const SearchOptions = {
            "formID": <DataProviderOptionInfo>{
                "display_name": "Form ID",
                "short_desc": "Get a specific form by ID",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "The ID of a specific form to retrieve",
            },
            "filter": <DataProviderOptionInfo>{
                "display_name": "Filter",
                "short_desc": "Filter conditions",
                "type": AbstractDataProviderType::get(AutoHashOrNothingType),
                "desc": "Filter forms using JotForm filter syntax (e.g., {\"status\": \"ENABLED\"})",
            },
            "orderby": <DataProviderOptionInfo>{
                "display_name": "Order By",
                "short_desc": "Sort order",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "Field to sort by (e.g., `created_at`, `updated_at`, `title`)",
            },
            "limit": <DataProviderOptionInfo>{
                "display_name": "Limit",
                "short_desc": "Maximum number of results",
                "type": AbstractDataProviderType::get(IntOrNothingType),
                "desc": "Maximum number of forms to return",
            },
        };
    }

    private {
        #! Record type
        static JotformFormDataType recordType();
    }

    #! Creates the object with a REST client
    constructor(JotformRestClient::JotformRestClient rest) : JotformDataProviderBase(rest) {
    }

    #! Creates the object with a connection
    constructor(JotformRestClient::JotformRestConnection conn) : JotformDataProviderBase(conn) {
    }

    #! Returns the provider name
    string getName() {
        return "forms";
    }

    #! Returns the provider description
    *string getDesc() {
        return "JotForm forms collection data provider";
    }

    #! Returns the record type
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return recordType.getFields();
    }

    #! Returns child provider names
    private *list<string> getChildProviderNamesImpl() {
        # Forms are accessed by ID directly
        return ();
    }

    #! Returns a child provider by name (form ID)
    private *AbstractDataProvider getChildProviderImpl(string name) {
        @assert(name.val(), "form ID cannot be empty");
        return new JotformFormDataProvider(rest, name);
    }

    #! Returns True, as any form ID can be a child
    private bool hasChildProviderImpl(string name) {
        return True;
    }

    #! Searches for forms
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        # If a specific form ID is requested, get just that form
        if (search_options.formID) {
            @debug(log(LoggerLevel::DEBUG, "searchRecordsImpl: getting single form %s", search_options.formID));
            hash<auto> response = doGet(sprintf("/form/%s", search_options.formID));
            list<hash<auto>> records;
            if (response.content) {
                records = (response.content,);
            }
            return new DefaultRecordIterator(new ListHashIterator(records), where_cond, search_options,
                recordType.getFields());
        }

        hash<auto> params;

        # Build query parameters from search options
        if (search_options.filter) {
%ifdef NoJson
            throw "JSON-ERROR", "the json module is required for filter support";
%else
            params.filter = make_json(search_options.filter);
%endif
        }
        if (search_options.orderby) {
            params.orderby = search_options.orderby;
        }

        int pageSize = DefaultPageSize;
        int totalLimit = search_options.limit ?? 0;

        @debug(log(LoggerLevel::DEBUG, "searchRecordsImpl: params=%y totalLimit=%d", params, totalLimit));

        return new JotformRecordIterator(rest, "/user/forms", params, pageSize,
            recordType.getFields(), totalLimit);
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace JotformDataProvider
