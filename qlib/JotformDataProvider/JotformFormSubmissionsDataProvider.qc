# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore JotformDataProvider module definition

/*  JotformFormSubmissionsDataProvider.qc Copyright 2025 - 2026 Qore Technologies, s.r.o.

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

#! JotForm form submissions collection data provider
public class JotformFormSubmissionsDataProvider inherits JotformDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "submissions",
            "desc": "JotForm form submissions data provider",
            "type": "JotformFormSubmissionsDataProvider",
            "supports_read": True,
            "supports_create": True,
            "supports_children": True,
            "supports_native_search": True,
            "has_record": True,
            "children_can_support_records": True,
            "search_options": SearchOptions,
            "create_options": CreateOptions,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions + {
            "formID": <DataProviderOptionInfo>{
                "display_name": "Form ID",
                "short_desc": "The form ID",
                "type": AbstractDataProviderType::get(StringType),
                "desc": "The ID of the form to get submissions from",
                "required": True,
            },
        };

        #! Search options
        const SearchOptions = {
            "submissionID": <DataProviderOptionInfo>{
                "display_name": "Submission ID",
                "short_desc": "Get a specific submission by ID",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "The unique ID of the submission to retrieve directly (bypasses formID requirement)",
            },
            "formID": <DataProviderOptionInfo>{
                "display_name": "Form",
                "short_desc": "The form to get submissions from",
                "type": AbstractDataProviderType::get(StringType),
                "desc": "The ID of the form to get submissions from",
                "required": True,
            },
            "filter": <DataProviderOptionInfo>{
                "display_name": "Filter",
                "short_desc": "Filter conditions",
                "type": AbstractDataProviderType::get(AutoHashOrNothingType),
                "desc": "Filter submissions using JotForm filter syntax (e.g., {\"status\": \"ACTIVE\"})",
            },
            "orderby": <DataProviderOptionInfo>{
                "display_name": "Order By",
                "short_desc": "Sort order",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "Field to sort by (e.g., `created_at`, `updated_at`)",
            },
            "limit": <DataProviderOptionInfo>{
                "display_name": "Limit",
                "short_desc": "Maximum number of results",
                "type": AbstractDataProviderType::get(IntOrNothingType),
                "desc": "Maximum number of submissions to return",
            },
        };

        #! Create options
        const CreateOptions = {};
    }

    private {
        #! The form ID (optional - can be set or accessed via parent)
        *string formID;

        #! Record type
        static JotformSubmissionDataType recordType();
    }

    #! Creates the object with a REST client
    constructor(JotformRestClient::JotformRestClient rest, *string formID) : JotformDataProviderBase(rest) {
        self.formID = formID;
    }

    #! Creates the object with a connection
    constructor(JotformRestClient::JotformRestConnection conn, *string formID) : JotformDataProviderBase(conn) {
        self.formID = formID;
    }

    #! Returns the provider name
    string getName() {
        return "submissions";
    }

    #! Returns the provider description
    *string getDesc() {
        if (formID) {
            return sprintf("JotForm submissions for form %s", formID);
        }
        return "JotForm submissions data provider (requires formID option)";
    }

    #! Returns the record type
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return recordType.getFields();
    }

    #! Returns child provider names
    private *list<string> getChildProviderNamesImpl() {
        # Submissions are accessed by ID directly
        return ();
    }

    #! Returns a child provider by name (submission ID)
    private *AbstractDataProvider getChildProviderImpl(string name) {
        @assert(name.val(), "submission ID cannot be empty");
        return new JotformSubmissionDataProvider(rest, name);
    }

    #! Returns True, as any submission ID can be a child
    private bool hasChildProviderImpl(string name) {
        return True;
    }

    #! Searches for submissions
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        # Handle single-record lookup by submission ID
        if (search_options.submissionID) {
            hash<auto> response = doGet(sprintf("/submission/%s", search_options.submissionID));
            list<hash<auto>> records;
            if (response.content) {
                records = (response.content,);
            }
            return new DefaultRecordIterator(new ListHashIterator(records), where_cond, search_options,
                recordType.getFields());
        }

        # Check for formID in search options if not set
        string form_id = formID ?? search_options.formID;
        @assert(form_id.val(), "formID is required to search submissions");

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

        @debug(log(LoggerLevel::DEBUG, "searchRecordsImpl: formID=%y params=%y totalLimit=%d",
            form_id, params, totalLimit));

        return new JotformRecordIterator(rest, sprintf("/form/%s/submissions", form_id), params, pageSize,
            recordType.getFields(), totalLimit);
    }

    #! Returns the record type for create operations
    private *hash<string, AbstractDataField> getCreateRecordTypeImpl(*hash<auto> search_options) {
        return (new JotformSubmissionCreateDataType()).getFields();
    }

    #! Creates a new submission
    /** JotForm expects submission data in form-encoded format with keys like:
        - submission[qid] for simple fields
        - submission[qid_first], submission[qid_last] for composite fields

        @see https://api.jotform.com/docs/#post-form-id-submissions
    */
    private *hash<auto> createRecordImpl(hash<auto> rec, *hash<auto> create_options) {
        # Use formID from options if not set
        string form_id = formID ?? rec.formID;
        @assert(form_id.val(), "formID is required to create a submission");
        @assert(rec.submission.typeCode() == NT_HASH, "submission data must be a hash");

        @debug(log(LoggerLevel::DEBUG, "createRecordImpl: creating submission for form %s with raw data: %y",
            form_id, rec.submission));

        # Transform to JotForm's expected format: submission[qid_subfield] = value
        hash<auto> transformed_data = transformSubmissionData(rec.submission, form_id);

        @debug(log(LoggerLevel::DEBUG, "createRecordImpl: transformed data: %y", transformed_data));

        # JotForm requires form-encoded POST for submissions, not JSON
        hash<auto> response = doPostFormEncoded(sprintf("/form/%s/submissions", form_id), transformed_data);

        @debug(log(LoggerLevel::DEBUG, "createRecordImpl: created submission, response=%y", response));

        # Response contains the new submission ID - extract it
        *string submission_id;
        if (response.content) {
            if (response.content.typeCode() == NT_STRING) {
                # Old format: plain string ID
                submission_id = response.content;
            } else if (response.content.typeCode() == NT_HASH) {
                # New format: {submissionID: "...", URL: "..."}
                submission_id = response.content.submissionID;
            }
        }

        # Fetch the created submission to return full data
        if (submission_id) {
            hash<auto> getResponse = doGet(sprintf("/submission/%s", submission_id));
            return getResponse.content;
        }

        # Return whatever content we got if we couldn't extract the ID
        return response.content;
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace JotformDataProvider
