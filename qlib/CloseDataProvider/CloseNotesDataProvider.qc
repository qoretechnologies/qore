# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file CloseNotesDataProvider.qc Close notes collection data provider

/*  CloseNotesDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Close notes collection data provider
public class CloseNotesDataProvider inherits CloseDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "notes",
            "desc": "Close notes data provider for listing and searching note activities",
            "type": "CloseNotesDataProvider",
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
            "note_id": <DataProviderOptionInfo>{
                "display_name": "Note ID",
                "short_desc": "Get a specific note by ID",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter by note ID for single record lookup",
            },
            "lead_id": <DataProviderOptionInfo>{
                "display_name": "Lead ID",
                "short_desc": "Filter by lead ID",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter notes by lead ID",
            },
            "user_id": <DataProviderOptionInfo>{
                "display_name": "User ID",
                "short_desc": "Filter by user ID",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter notes by user ID",
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
            "create": Class::forName("CloseCreateNoteDataProvider"),
            "update": Class::forName("CloseUpdateNoteDataProvider"),
            "delete": Class::forName("CloseDeleteNoteDataProvider"),
        };
    }

    #! Creates the object with a REST client
    constructor(CloseRestClient::CloseRestClient rest) : CloseDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "notes";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Close notes data provider for listing and searching note activities";
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
        return new CloseNoteDataType();
    }

    #! Returns an iterator for records matching the search criteria
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        if (search_options.note_id) {
            hash<auto> result = doGet("activity/note/" + search_options.note_id);
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

        list<hash<auto>> all_records = fetchAllPages("activity/note", params,
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
