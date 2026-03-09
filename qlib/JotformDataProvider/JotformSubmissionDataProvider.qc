# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore JotformDataProvider module definition

/*  JotformSubmissionDataProvider.qc Copyright 2025 - 2026 Qore Technologies, s.r.o.

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

#! JotForm single submission data provider
public class JotformSubmissionDataProvider inherits JotformDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "submission",
            "desc": "JotForm single submission data provider",
            "type": "JotformSubmissionDataProvider",
            "supports_read": True,
            "supports_update": True,
            "supports_delete": True,
            "has_record": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions + {
            "submissionID": <DataProviderOptionInfo>{
                "display_name": "Submission ID",
                "short_desc": "The submission ID",
                "type": AbstractDataProviderType::get(StringType),
                "desc": "The ID of the submission",
                "required": True,
            },
        };
    }

    private {
        #! The submission ID
        string submissionID;

        #! Record type
        static JotformSubmissionDataType recordType();
    }

    #! Creates the object with a REST client
    constructor(JotformRestClient::JotformRestClient rest, string submissionID) : JotformDataProviderBase(rest) {
        @assert(submissionID.val(), "submission ID cannot be empty");
        self.submissionID = submissionID;
    }

    #! Creates the object with a connection
    constructor(JotformRestClient::JotformRestConnection conn, string submissionID) : JotformDataProviderBase(conn) {
        @assert(submissionID.val(), "submission ID cannot be empty");
        self.submissionID = submissionID;
    }

    #! Returns the provider name
    string getName() {
        return submissionID;
    }

    #! Returns the provider description
    *string getDesc() {
        return sprintf("JotForm submission %s", submissionID);
    }

    #! Returns the record type
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return recordType.getFields();
    }

    #! Returns the record type for update operations
    private *hash<string, AbstractDataField> getUpdateRecordTypeImpl(*hash<auto> search_options) {
        return (new JotformSubmissionUpdateDataType()).getFields();
    }

    #! Searches for the submission (returns single record)
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        hash<auto> response = doGet(sprintf("/submission/%s", submissionID));
        list<hash<auto>> records;
        if (response.content) {
            records = (response.content,);
        }
        return new DefaultRecordIterator(new ListHashIterator(records), where_cond, search_options,
            recordType.getFields());
    }

    #! Updates the submission
    /** @param set the fields to update
        @param where_cond ignored (ID is already set)
        @param search_options ignored

        @return the number of records updated (0 or 1)

        JotForm expects submission data in form-encoded format with keys like:
        - submission[qid] for simple fields
        - submission[qid_first], submission[qid_last] for composite fields

        @see https://api.jotform.com/docs/#post-submission-id
    */
    private int updateRecordsImpl(hash<auto> set, *hash<auto> where_cond, *hash<auto> search_options) {
        @debug(log(LoggerLevel::DEBUG, "updateRecordsImpl: updating submission %s with raw data: %y", submissionID, set));

        # Get formID from the submission to transform field names
        hash<auto> submission_info = doGet(sprintf("/submission/%s", submissionID));
        string form_id = submission_info.content.form_id;
        @assert(form_id.val(), "Could not determine form ID for submission");

        # Transform to JotForm's expected format: submission[qid_subfield] = value
        hash<auto> transformed_data = transformSubmissionData(set, form_id);

        @debug(log(LoggerLevel::DEBUG, "updateRecordsImpl: transformed data: %y", transformed_data));

        # JotForm requires form-encoded POST for submissions, not JSON
        hash<auto> response = doPostFormEncoded(sprintf("/submission/%s", submissionID), transformed_data);

        @debug(log(LoggerLevel::DEBUG, "updateRecordsImpl: update complete, response=%y", response));
        return 1;
    }

    #! Updates the single submission
    /** @param set the fields to update
        @param search_options ignored

        @return True if the record was updated
    */
    bool updateSingleRecord(hash<auto> set, *hash<auto> search_options) {
        return updateRecordsImpl(set) == 1;
    }

    #! Deletes the submission
    /** @param where_cond ignored (ID is already set)
        @param search_options ignored

        @return the number of records deleted (0 or 1)
    */
    private int deleteRecordsImpl(*hash<auto> where_cond, *hash<auto> search_options) {
        @debug(log(LoggerLevel::DEBUG, "deleteRecordsImpl: deleting submission %s", submissionID));

        hash<auto> response = doDelete(sprintf("/submission/%s", submissionID));

        @debug(log(LoggerLevel::DEBUG, "deleteRecordsImpl: delete complete, response=%y", response));
        return 1;
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace JotformDataProvider
