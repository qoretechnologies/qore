# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore LinearDataProvider module definition

/*  LinearCyclesDataProvider.qc Copyright 2025 Qore Technologies, s.r.o.

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

#! Linear cycles collection data provider
public class LinearCyclesDataProvider inherits LinearDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "cycles",
            "desc": "Linear cycles (sprints) data provider",
            "type": "LinearCyclesDataProvider",
            "supports_read": True,
            "supports_create": True,
            "supports_children": True,
            "supports_native_search": True,
            "has_record": True,
            "children_can_support_records": True,
            "search_options": SearchOptions,
        };

        #! Search options
        const SearchOptions = {
            "teamId": <DataProviderOptionInfo>{
                "display_name": "Team ID",
                "short_desc": "Filter by team ID",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "Filter cycles by team ID",
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
        static LinearCycleDataType recordType();
    }

    #! Creates the object with a REST client
    constructor(LinearRestClient::LinearRestClient rest) : LinearDataProviderBase(rest) {
    }

    #! Creates the object with a connection
    constructor(LinearRestClient::LinearRestConnection conn) : LinearDataProviderBase(conn) {
    }

    #! Returns the provider name
    string getName() {
        return "cycles";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Linear cycles (sprints) collection data provider";
    }

    #! Returns the record type
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return recordType.getFields();
    }

    #! Returns child provider names
    private *list<string> getChildProviderNamesImpl() {
        return ();
    }

    #! Returns a child provider by name (cycle ID)
    private *AbstractDataProvider getChildProviderImpl(string name) {
        @assert(name.val(), "cycle ID cannot be empty");
        return new LinearCycleDataProvider(rest, name);
    }

    #! Returns True, as any cycle ID can be a child
    private bool hasChildProviderImpl(string name) {
        return True;
    }

    #! Searches for cycles
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        hash<auto> filter;
        if (search_options.teamId) {
            filter.team = {"id": {"eq": search_options.teamId}};
        }

        hash<auto> vars;
        if (filter) {
            vars.filter = filter;
        }
        @debug(log(LoggerLevel::DEBUG, "searchRecordsImpl: filter=%y", filter));

        int pageSize = search_options.limit ?? DefaultPageSize;

        return new LinearRecordIterator(rest, ListCyclesQuery, "cycles", vars, pageSize,
            recordType.getFields());
    }

    #! Creates a new cycle
    private *hash<auto> createRecordImpl(hash<auto> rec, *hash<auto> create_options) {
        @assert(rec.teamId.val(), "teamId is required to create a cycle");
        @debug(log(LoggerLevel::DEBUG, "createRecordImpl: creating cycle for teamId=%y", rec.teamId));
        hash<auto> data = doGraphQL(CreateCycleMutation, {"input": rec});
        @assert(data.cycleCreate, "cycleCreate response cannot be null");
        if (!data.cycleCreate.success) {
            throw "CREATE-ERROR", "Failed to create cycle";
        }
        @assert(data.cycleCreate.cycle, "created cycle cannot be null on success");
        @debug(log(LoggerLevel::DEBUG, "createRecordImpl: created cycle id=%y", data.cycleCreate.cycle.id));
        return data.cycleCreate.cycle;
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace LinearDataProvider
