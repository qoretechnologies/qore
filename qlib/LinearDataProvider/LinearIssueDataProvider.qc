# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore LinearDataProvider module definition

/*  LinearIssueDataProvider.qc Copyright 2025 - 2026 Qore Technologies, s.r.o.

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

#! Linear single issue data provider
public class LinearIssueDataProvider inherits LinearDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "LinearIssueDataProvider",
            "supports_read": True,
            "supports_update": True,
            "supports_delete": True,
            "supports_children": True,
            "has_record": True,
            "children_can_support_records": True,
        };

        #! Child provider map
        const ChildMap = {
            "comments": Class::forName("LinearCommentsDataProvider"),
        };
    }

    private {
        #! The issue ID
        string issueId;

        #! Record type
        static LinearIssueDataType recordType();
    }

    #! Creates the object
    constructor(LinearRestClient::LinearRestClient rest, string issueId) : LinearDataProviderBase(rest) {
        @assert(issueId.val(), "issue ID cannot be empty");
        self.issueId = issueId;
    }

    #! Returns the provider name
    string getName() {
        return issueId;
    }

    #! Returns the provider description
    *string getDesc() {
        return sprintf("Linear issue %s data provider", issueId);
    }

    #! Returns the record type
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return recordType.getFields();
    }

    #! Returns child provider names
    private *list<string> getChildProviderNamesImpl() {
        return keys ChildMap;
    }

    #! Returns a child provider by name
    private *AbstractDataProvider getChildProviderImpl(string name) {
        *Class cls = ChildMap{name};
        if (cls) {
            return cls.newObject(rest, issueId);
        }
    }

    #! Searches for the issue record (returns single record iterator)
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        @debug(log(LoggerLevel::DEBUG, "searchRecordsImpl: fetching issue id=%y", issueId));
        hash<auto> data = doGraphQL(GetIssueQuery, {"id": issueId});
        @debug(log(LoggerLevel::DEBUG, "searchRecordsImpl: issue found=%y", exists data.issue));
        return new DefaultRecordIterator(new ListHashIterator(data.issue ? (data.issue,) : ()),
            where_cond, search_options, recordType.getFields());
    }

    #! Updates the issue
    private bool updateSingleRecordImpl(hash<auto> set, hash<auto> where_cond,
            *hash<auto> search_options) {
        @assert(set, "update data cannot be empty");
        @debug(log(LoggerLevel::DEBUG, "updateSingleRecordImpl: updating issue id=%y fields=%y", issueId, keys set));
        hash<auto> data = doGraphQL(UpdateIssueMutation, {"id": issueId, "input": set});
        @assert(data.issueUpdate, "issueUpdate response cannot be null");
        if (!data.issueUpdate.success) {
            throw "UPDATE-ERROR", "Failed to update issue";
        }
        @debug(log(LoggerLevel::DEBUG, "updateSingleRecordImpl: issue updated successfully"));
        return True;
    }

    #! Archives (soft-deletes) the issue
    private int deleteRecordsImpl(*hash<auto> where_cond, *hash<auto> search_options) {
        @debug(log(LoggerLevel::DEBUG, "deleteRecordsImpl: archiving issue id=%y", issueId));
        hash<auto> data = doGraphQL(ArchiveIssueMutation, {"id": issueId});
        @assert(data.issueArchive, "issueArchive response cannot be null");
        @debug(log(LoggerLevel::DEBUG, "deleteRecordsImpl: archive success=%y", data.issueArchive.success));
        return data.issueArchive.success ? 1 : 0;
    }

    #! Returns example data for the record type without making API calls
    private hash<auto> getExampleRecordDataImpl() {
        return generateExampleData(recordType.getFields());
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return cast<hash<DataProviderInfo>>(ProviderInfo + {
            "name": issueId,
            "desc": sprintf("Linear issue %s", issueId),
        });
    }
}

} # namespace LinearDataProvider
