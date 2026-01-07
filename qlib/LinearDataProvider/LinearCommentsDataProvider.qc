# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore LinearDataProvider module definition

/*  LinearCommentsDataProvider.qc Copyright 2025 Qore Technologies, s.r.o.

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

#! Linear comments collection data provider (child of issue)
public class LinearCommentsDataProvider inherits LinearDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "comments",
            "desc": "Linear comments data provider",
            "type": "LinearCommentsDataProvider",
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
            "limit": <DataProviderOptionInfo>{
                "display_name": "Limit",
                "short_desc": "Maximum number of results",
                "type": AbstractDataProviderType::get(IntOrNothingType),
                "desc": "Maximum number of results to return",
            },
        };
    }

    private {
        #! The issue ID this comments provider is for
        string issueId;

        #! Record type
        static LinearCommentDataType recordType();
    }

    #! Creates the object with a REST client and issue ID
    constructor(LinearRestClient::LinearRestClient rest, string issueId) : LinearDataProviderBase(rest) {
        @assert(issueId.val(), "issue ID cannot be empty");
        self.issueId = issueId;
    }

    #! Returns the provider name
    string getName() {
        return "comments";
    }

    #! Returns the provider description
    *string getDesc() {
        return sprintf("Linear comments for issue %s", issueId);
    }

    #! Returns the record type
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return recordType.getFields();
    }

    #! Returns child provider names
    private *list<string> getChildProviderNamesImpl() {
        return ();
    }

    #! Returns a child provider by name (comment ID)
    private *AbstractDataProvider getChildProviderImpl(string name) {
        @assert(name.val(), "comment ID cannot be empty");
        return new LinearCommentDataProvider(rest, name);
    }

    #! Returns True, as any comment ID can be a child
    private bool hasChildProviderImpl(string name) {
        return True;
    }

    #! Searches for comments on this issue
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        @debug(log(LoggerLevel::DEBUG, "searchRecordsImpl: fetching comments for issue id=%y", issueId));
        int pageSize = search_options.limit ?? DefaultPageSize;

        hash<auto> vars = {
            "issueId": issueId,
        };

        return new LinearRecordIterator(rest, ListCommentsQuery, "issue", "comments",
            vars, pageSize, recordType.getFields());
    }

    #! Creates a new comment on this issue
    private *hash<auto> createRecordImpl(hash<auto> rec, *hash<auto> create_options) {
        @assert(rec.body.val(), "comment body is required");
        @debug(log(LoggerLevel::DEBUG, "createRecordImpl: creating comment on issue id=%y", issueId));
        # Add the issue ID to the input
        hash<auto> input = rec + {"issueId": issueId};
        hash<auto> data = doGraphQL(CreateCommentMutation, {"input": input});
        @assert(data.commentCreate, "commentCreate response cannot be null");
        if (!data.commentCreate.success) {
            throw "CREATE-ERROR", "Failed to create comment";
        }
        @assert(data.commentCreate.comment, "created comment cannot be null on success");
        @debug(log(LoggerLevel::DEBUG, "createRecordImpl: created comment id=%y", data.commentCreate.comment.id));
        return data.commentCreate.comment;
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace LinearDataProvider
