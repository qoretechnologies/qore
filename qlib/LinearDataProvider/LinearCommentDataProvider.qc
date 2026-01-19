# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore LinearDataProvider module definition

/*  LinearCommentDataProvider.qc Copyright 2025 Qore Technologies, s.r.o.

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

#! Linear single comment data provider
public class LinearCommentDataProvider inherits LinearDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "LinearCommentDataProvider",
            "supports_read": True,
            "supports_update": True,
            "supports_delete": True,
            "has_record": True,
        };
    }

    private {
        #! The comment ID
        string commentId;

        #! Record type
        static LinearCommentDataType recordType();
    }

    #! Creates the object
    constructor(LinearRestClient::LinearRestClient rest, string commentId) : LinearDataProviderBase(rest) {
        @assert(commentId.val(), "comment ID cannot be empty");
        self.commentId = commentId;
    }

    #! Returns the provider name
    string getName() {
        return commentId;
    }

    #! Returns the provider description
    *string getDesc() {
        return sprintf("Linear comment %s data provider", commentId);
    }

    #! Returns the record type
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return recordType.getFields();
    }

    #! Searches for the comment record (returns single record iterator)
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        @debug(log(LoggerLevel::DEBUG, "searchRecordsImpl: fetching comment id=%y", commentId));
        hash<auto> data = doGraphQL(GetCommentQuery, {"id": commentId});
        @debug(log(LoggerLevel::DEBUG, "searchRecordsImpl: comment found=%y", exists data.comment));
        return new DefaultRecordIterator(new ListHashIterator(data.comment ? (data.comment,) : ()),
            where_cond, search_options, recordType.getFields());
    }

    #! Updates the comment
    private bool updateSingleRecordImpl(hash<auto> set, hash<auto> where_cond,
            *hash<auto> search_options) {
        @assert(set, "update data cannot be empty");
        @debug(log(LoggerLevel::DEBUG, "updateSingleRecordImpl: updating comment id=%y fields=%y", commentId,
            keys set));
        hash<auto> data = doGraphQL(UpdateCommentMutation, {"id": commentId, "input": set});
        @assert(data.commentUpdate, "commentUpdate response cannot be null");
        if (!data.commentUpdate.success) {
            throw "UPDATE-ERROR", "Failed to update comment";
        }
        @debug(log(LoggerLevel::DEBUG, "updateSingleRecordImpl: comment updated successfully"));
        return True;
    }

    #! Deletes the comment (hard delete, not archive)
    private int deleteRecordsImpl(*hash<auto> where_cond, *hash<auto> search_options) {
        @debug(log(LoggerLevel::DEBUG, "deleteRecordsImpl: deleting comment id=%y", commentId));
        hash<auto> data = doGraphQL(DeleteCommentMutation, {"id": commentId});
        @assert(data.commentDelete, "commentDelete response cannot be null");
        @debug(log(LoggerLevel::DEBUG, "deleteRecordsImpl: delete success=%y", data.commentDelete.success));
        return data.commentDelete.success ? 1 : 0;
    }

    #! Returns example data for the record type without making API calls
    private hash<auto> getExampleRecordDataImpl() {
        return generateExampleData(recordType.getFields());
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return cast<hash<DataProviderInfo>>(ProviderInfo + {
            "name": commentId,
            "desc": sprintf("Linear comment %s", commentId),
        });
    }
}

} # namespace LinearDataProvider
