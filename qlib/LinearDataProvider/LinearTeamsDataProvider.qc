# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore LinearDataProvider module definition

/*  LinearTeamsDataProvider.qc Copyright 2025 - 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the LinearDataProvider module
public namespace LinearDataProvider {

#! Linear teams collection data provider (read-only)
public class LinearTeamsDataProvider inherits LinearDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "teams",
            "desc": "Linear teams data provider (read-only)",
            "type": "LinearTeamsDataProvider",
            "supports_read": True,
            "supports_children": True,
            "supports_native_search": True,
            "has_record": True,
            "children_can_support_records": True,
            "search_options": SearchOptions,
        };

        #! Search options
        const SearchOptions = {
            "id": <DataProviderOptionInfo>{
                "display_name": "Team ID",
                "short_desc": "Get a specific team by ID",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "The unique ID of the team to retrieve",
            },
            "limit": <DataProviderOptionInfo>{
                "display_name": "Limit",
                "short_desc": "Maximum number of results",
                "type": AbstractDataProviderType::get(IntOrNothingType),
                "desc": "Maximum number of results to return",
            },
        };
    }

    private {
        #! Record type
        static LinearTeamDataType recordType();
    }

    #! Creates the object with a REST client
    constructor(LinearRestClient::LinearRestClient rest) : LinearDataProviderBase(rest) {
    }

    #! Creates the object with a connection
    constructor(LinearRestClient::LinearRestConnection conn) : LinearDataProviderBase(conn) {
    }

    #! Returns the provider name
    string getName() {
        return "teams";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Linear teams collection data provider (read-only)";
    }

    #! Returns the record type
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return recordType.getFields();
    }

    #! Returns child provider names
    private *list<string> getChildProviderNamesImpl() {
        return ();
    }

    #! Returns a child provider by name (team ID)
    private *AbstractDataProvider getChildProviderImpl(string name) {
        @assert(name.val(), "team ID cannot be empty");
        return new LinearTeamDataProvider(rest, name);
    }

    #! Returns True, as any team ID can be a child
    private bool hasChildProviderImpl(string name) {
        return True;
    }

    #! Searches for teams
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        # Handle single-record lookup by ID
        if (search_options.id) {
            hash<auto> data = doGraphQL(GetTeamQuery, {"id": search_options.id});
            return new DefaultRecordIterator(new ListHashIterator(data.team
                ? (data.team,) : ()), where_cond, search_options,
                recordType.getFields());
        }

        @debug(log(LoggerLevel::DEBUG, "searchRecordsImpl: fetching teams"));
        int pageSize = search_options.limit ?? DefaultPageSize;
        return new LinearRecordIterator(rest, ListTeamsQuery, "teams", NOTHING, pageSize,
            recordType.getFields());
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace LinearDataProvider
