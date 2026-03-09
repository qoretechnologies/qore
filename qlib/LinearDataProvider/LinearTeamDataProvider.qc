# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore LinearDataProvider module definition

/*  LinearTeamDataProvider.qc Copyright 2025 - 2026 Qore Technologies, s.r.o.

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

#! Linear single team data provider (read-only)
public class LinearTeamDataProvider inherits LinearDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "LinearTeamDataProvider",
            "supports_read": True,
            "has_record": True,
        };
    }

    private {
        #! The team ID
        string teamId;

        #! Record type
        static LinearTeamDataType recordType();
    }

    #! Creates the object
    constructor(LinearRestClient::LinearRestClient rest, string teamId) : LinearDataProviderBase(rest) {
        @assert(teamId.val(), "team ID cannot be empty");
        self.teamId = teamId;
    }

    #! Returns the provider name
    string getName() {
        return teamId;
    }

    #! Returns the provider description
    *string getDesc() {
        return sprintf("Linear team %s data provider (read-only)", teamId);
    }

    #! Returns the record type
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return recordType.getFields();
    }

    #! Searches for the team record (returns single record iterator)
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        @debug(log(LoggerLevel::DEBUG, "searchRecordsImpl: fetching team id=%y", teamId));
        hash<auto> data = doGraphQL(GetTeamQuery, {"id": teamId});
        @debug(log(LoggerLevel::DEBUG, "searchRecordsImpl: team found=%y", exists data.team));
        return new DefaultRecordIterator(new ListHashIterator(data.team ? (data.team,) : ()),
            where_cond, search_options, recordType.getFields());
    }

    #! Returns example data for the record type without making API calls
    private hash<auto> getExampleRecordDataImpl() {
        return generateExampleData(recordType.getFields());
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return cast<hash<DataProviderInfo>>(ProviderInfo + {
            "name": teamId,
            "desc": sprintf("Linear team %s (read-only)", teamId),
        });
    }
}

} # namespace LinearDataProvider
