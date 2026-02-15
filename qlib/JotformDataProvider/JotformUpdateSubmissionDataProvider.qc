# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore JotformDataProvider module definition

/*  JotformUpdateSubmissionDataProvider.qc Copyright 2025 Qore Technologies, s.r.o.

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

#! Data provider for updating JotForm submissions via request API
public class JotformUpdateSubmissionDataProvider inherits JotformDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "update-submission",
            "desc": "Update an existing submission in JotForm",
            "type": "JotformUpdateSubmissionDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Request type - the submission data to update
        static JotformSubmissionUpdateDataType RequestType();

        #! Response type - the updated submission
        static JotformSubmissionDataType ResponseType();
    }

    #! Creates the object with a REST client
    constructor(JotformRestClient::JotformRestClient rest) : JotformDataProviderBase(rest) {
    }

    #! Creates the object with a connection
    constructor(JotformRestClient::JotformRestConnection conn) : JotformDataProviderBase(conn) {
    }

    #! Returns the provider name
    string getName() {
        return "update-submission";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Update an existing submission in JotForm";
    }

    #! Makes a request and returns the response
    /** @param req the request data (submission ID and fields to update)
        @param request_options the request options

        @return the updated submission data

        The request contains formID, submissionID and all form fields at the top level.
        We extract submissionID and send the remaining fields (except formID) as submission data.

        JotForm expects submission data in form-encoded format with keys like:
        - submission[qid] for simple fields
        - submission[qid_first], submission[qid_last] for composite fields

        @see https://api.jotform.com/docs/#post-submission-id
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        @assert(req.submissionID.val(), "submissionID is required to update a submission");

        # Extract plain value if submissionID comes from ref_data
        string submission_id = req.submissionID.value ?? req.submissionID;

        # Get formID - required for transforming field names to qids
        *string form_id = req.formID.value ?? req.formID;

        # If formID not provided, get it from the submission
        if (!form_id) {
            hash<auto> submission_info = doGet(sprintf("/submission/%s", submission_id));
            form_id = submission_info.content.form_id;
        }
        @assert(form_id.val(), "formID is required to update a submission (either provide it or it will be fetched from the submission)");

        # Extract submission data - everything except formID and submissionID
        hash<auto> submission_data = req - ("formID", "submissionID");

        # Handle legacy format where submission data is nested
        if (submission_data.submission && submission_data.size() == 1) {
            submission_data = submission_data.submission;
        }

        @debug(log(LoggerLevel::DEBUG, "doRequestImpl: updating submission %s with raw data: %y",
            submission_id, submission_data));

        # Transform to JotForm's expected format: submission[qid_subfield] = value
        hash<auto> transformed_data = transformSubmissionData(submission_data, form_id);

        @debug(log(LoggerLevel::DEBUG, "doRequestImpl: transformed data: %y", transformed_data));

        # JotForm requires form-encoded POST for submissions, not JSON
        hash<auto> response = doPostFormEncoded(sprintf("/submission/%s", submission_id), transformed_data);

        @debug(log(LoggerLevel::DEBUG, "doRequestImpl: update response=%y", response));

        # Fetch the updated submission to return full data
        hash<auto> getResponse = doGet(sprintf("/submission/%s", submission_id));
        return getResponse.content;
    }

    #! Returns the description of a successful request message, if any
    /** @return the request type for this provider
    */
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return RequestType;
    }

    #! Returns the request type based on the actual request data
    /** @param req the request data containing formID
        @return a dynamic request type with form-specific fields

        This method is called during request validation. We extract formID from the
        request and return a dynamic type that allows the form-specific fields.
    */
    private *DataProvider::AbstractDataProviderType getRequestTypeWithDataImpl(auto req) {
        *string form_id;
        if (req.typeCode() == NT_HASH) {
            form_id = req.formID.value ?? req.formID;
        }
        if (!form_id) {
            return RequestType;
        }
        return buildDynamicRequestType(form_id);
    }

    #! Returns the request type with dynamic form fields based on formID option
    /** @param options the options hash containing formID
        @return a dynamic request type with form-specific fields
    */
    private *DataProvider::AbstractDataProviderType getRequestTypeWithOptionsImpl(*hash<auto> options) {
        *string form_id = options.formID.value ?? options.formID;
        if (!form_id) {
            # No form selected yet, return base type
            return RequestType;
        }

        # Build dynamic type with form-specific fields
        return buildDynamicRequestType(form_id);
    }

    #! Builds a dynamic request type with form-specific submission fields
    /** The form fields are returned at the top level of the request type, so they
        appear as individual options in the UI. This is the expected pattern for
        dynamic options - each field becomes its own option.
    */
    private AbstractDataProviderType buildDynamicRequestType(string form_id) {
        # Get form questions from API
        hash<auto> response = doGet(sprintf("/form/%s/questions", form_id));

        # Build fields hash starting with formID and submissionID
        hash<string, AbstractDataField> fields = {
            "formID": new QoreDataField({
                "name": "formID",
                "display_name": "Form",
                "short_desc": "The form containing the submission",
                "desc": "The form ID to get field definitions",
                "type": AbstractDataProviderTypeMap."string",
            }),
            "submissionID": new QoreDataField({
                "name": "submissionID",
                "display_name": "Submission ID",
                "short_desc": "The submission to update",
                "desc": "The ID of the submission to update",
                "type": AbstractDataProviderTypeMap."string",
            }),
        };

        # Add each form question as a top-level field
        if (response.content) {
            foreach string qid in (keys response.content) {
                hash<auto> question = response.content{qid};
                string field_name = question.name ?? qid;
                string display_name = question.text ?? field_name;
                AbstractDataProviderType field_type = getFieldTypeForQuestion(question);

                fields{field_name} = new QoreDataField({
                    "name": field_name,
                    "display_name": display_name,
                    "short_desc": question.type ?? "field",
                    "desc": sprintf("%s (Question ID: %s)", display_name, qid),
                    "type": field_type,
                });
            }
        }

        return new HashDataType("UpdateSubmissionRequest", fields);
    }

    #! Returns the appropriate Qore type for a JotForm question type
    /** Delegates to the base class static method for consistent type mapping.
    */
    private AbstractDataProviderType getFieldTypeForQuestion(hash<auto> question) {
        return JotformDataProviderBase::getFieldTypeForQuestion(question);
    }

    #! Returns the description of a response message, if this object represents a response message
    /** @return the response type for this response message
    */
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return ResponseType;
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace JotformDataProvider
